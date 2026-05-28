#include "bridge.h"
#include "bridge_compat.h"
#include "php_parallax.h"

#include <Zend/zend_API.h>
#include <Zend/zend_closures.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_smart_str.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ────────────────────────────────────────────────────────────────────────── */
/*  Source slice — extract the text of an inline closure from its origin file */
/* ────────────────────────────────────────────────────────────────────────── */

typedef struct {
	char  *data;
	size_t len;
} px_slice_t;

static int read_file_all(const char *path, px_slice_t *out)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		return -1;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	long sz = ftell(f);
	if (sz < 0) {
		fclose(f);
		return -1;
	}
	rewind(f);
	char *buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL) {
		fclose(f);
		return -1;
	}
	size_t got = fread(buf, 1, (size_t)sz, f);
	fclose(f);
	if (got != (size_t)sz) {
		free(buf);
		return -1;
	}
	buf[sz] = '\0';
	out->data = buf;
	out->len = (size_t)sz;
	return 0;
}

/* Find byte offset where `line_no` (1-indexed) starts. Returns -1 on error. */
static long offset_of_line(const px_slice_t *s, int line_no)
{
	if (line_no <= 1) {
		return 0;
	}
	int seen = 1;
	for (size_t i = 0; i < s->len; i++) {
		if (s->data[i] == '\n') {
			seen++;
			if (seen == line_no) {
				return (long)(i + 1);
			}
		}
	}
	return -1;
}

/* Walk forward through `src` looking for a literal token `keyword`, skipping
 * PHP single/double-quoted strings, heredoc/nowdoc strings, line and block
 * comments. Returns the offset where the keyword begins, or -1 if not found.
 * The match is whole-token: the bytes before and after must be non-identifier
 * characters. */
typedef enum {
	SCAN_NORMAL,
	SCAN_SQ_STRING,
	SCAN_DQ_STRING,
	SCAN_LINE_COMMENT,
	SCAN_BLOCK_COMMENT,
} scan_state_t;

static bool is_ident_byte(char c)
{
	return (c >= 'a' && c <= 'z')
	    || (c >= 'A' && c <= 'Z')
	    || (c >= '0' && c <= '9')
	    || c == '_';
}

static long scan_find_keyword(const px_slice_t *src, size_t from, const char *keyword)
{
	size_t klen = strlen(keyword);
	scan_state_t st = SCAN_NORMAL;
	char heredoc_label[64];
	size_t hd_len = 0;
	bool nowdoc = false;
	(void)nowdoc;

	for (size_t i = from; i < src->len; i++) {
		char c = src->data[i];
		char n = (i + 1 < src->len) ? src->data[i + 1] : '\0';

		switch (st) {
			case SCAN_NORMAL:
				if (c == '\'') { st = SCAN_SQ_STRING; break; }
				if (c == '"')  { st = SCAN_DQ_STRING; break; }
				if (c == '/' && n == '/') { st = SCAN_LINE_COMMENT; i++; break; }
				if (c == '#'  && n != '[') { st = SCAN_LINE_COMMENT; break; }
				if (c == '/' && n == '*') { st = SCAN_BLOCK_COMMENT; i++; break; }
				/* heredoc/nowdoc: <<<LABEL or <<<'LABEL' — minimal handling */
				if (c == '<' && n == '<' && i + 2 < src->len && src->data[i + 2] == '<') {
					size_t j = i + 3;
					while (j < src->len && (src->data[j] == ' ' || src->data[j] == '\t')) j++;
					if (j < src->len && src->data[j] == '\'') { nowdoc = true; j++; }
					hd_len = 0;
					while (j < src->len && is_ident_byte(src->data[j]) && hd_len + 1 < sizeof(heredoc_label)) {
						heredoc_label[hd_len++] = src->data[j++];
					}
					heredoc_label[hd_len] = '\0';
					if (nowdoc && j < src->len && src->data[j] == '\'') j++;
					/* skip to the closing label at start of a line — minimal: scan for "\nLABEL" */
					while (j < src->len) {
						if (src->data[j] == '\n') {
							size_t k = j + 1;
							/* allow indentation per PHP 7.3+ */
							while (k < src->len && (src->data[k] == ' ' || src->data[k] == '\t')) k++;
							if (k + hd_len <= src->len
								&& memcmp(src->data + k, heredoc_label, hd_len) == 0
								&& (k + hd_len == src->len || !is_ident_byte(src->data[k + hd_len]))) {
								j = k + hd_len;
								break;
							}
						}
						j++;
					}
					i = j;
					nowdoc = false;
					break;
				}
				/* keyword test */
				if (i + klen <= src->len
					&& memcmp(src->data + i, keyword, klen) == 0
					&& (i == 0 || !is_ident_byte(src->data[i - 1]))
					&& (i + klen == src->len || !is_ident_byte(src->data[i + klen]))) {
					return (long)i;
				}
				break;

			case SCAN_SQ_STRING:
				if (c == '\\' && n != '\0') { i++; break; }
				if (c == '\'') { st = SCAN_NORMAL; }
				break;
			case SCAN_DQ_STRING:
				if (c == '\\' && n != '\0') { i++; break; }
				if (c == '"') { st = SCAN_NORMAL; }
				break;
			case SCAN_LINE_COMMENT:
				if (c == '\n') { st = SCAN_NORMAL; }
				break;
			case SCAN_BLOCK_COMMENT:
				if (c == '*' && n == '/') { st = SCAN_NORMAL; i++; }
				break;
		}
	}
	return -1;
}

/* Brace-balance from `from` (which must point at an opening `{`). Returns the
 * offset *after* the matching `}` or -1 on imbalance. */
static long scan_brace_match(const px_slice_t *src, size_t from)
{
	if (from >= src->len || src->data[from] != '{') {
		return -1;
	}
	int depth = 0;
	scan_state_t st = SCAN_NORMAL;

	for (size_t i = from; i < src->len; i++) {
		char c = src->data[i];
		char n = (i + 1 < src->len) ? src->data[i + 1] : '\0';

		switch (st) {
			case SCAN_NORMAL:
				if (c == '\'') { st = SCAN_SQ_STRING; break; }
				if (c == '"')  { st = SCAN_DQ_STRING; break; }
				if (c == '/' && n == '/') { st = SCAN_LINE_COMMENT; i++; break; }
				if (c == '#'  && n != '[') { st = SCAN_LINE_COMMENT; break; }
				if (c == '/' && n == '*') { st = SCAN_BLOCK_COMMENT; i++; break; }
				if (c == '{') { depth++; break; }
				if (c == '}') {
					depth--;
					if (depth == 0) {
						return (long)(i + 1);
					}
					break;
				}
				break;
			case SCAN_SQ_STRING:
				if (c == '\\' && n != '\0') { i++; break; }
				if (c == '\'') { st = SCAN_NORMAL; }
				break;
			case SCAN_DQ_STRING:
				if (c == '\\' && n != '\0') { i++; break; }
				if (c == '"') { st = SCAN_NORMAL; }
				break;
			case SCAN_LINE_COMMENT:
				if (c == '\n') { st = SCAN_NORMAL; }
				break;
			case SCAN_BLOCK_COMMENT:
				if (c == '*' && n == '/') { st = SCAN_NORMAL; i++; }
				break;
		}
	}
	return -1;
}

/* Slice the source of an inline closure starting somewhere on `line_start`.
 * Returns malloc'd text such as `function ($a) use ($x) { ... }`, or NULL.
 * Sets *err with a human-readable cause on failure. */
static char *extract_closure_text(const char *filename, int line_start, char **err)
{
	*err = NULL;

	if (filename == NULL || strstr(filename, "Standard input") != NULL
		|| strncmp(filename, "eval", 4) == 0
		|| strstr(filename, "://") != NULL) {
		*err = strdup("closure has no resolvable source file (eval / repl / stream wrapper)");
		return NULL;
	}

	px_slice_t src;
	if (read_file_all(filename, &src) != 0) {
		*err = strdup("could not read closure source file");
		return NULL;
	}

	long line_off = offset_of_line(&src, line_start);
	if (line_off < 0) {
		free(src.data);
		*err = strdup("closure line number is out of range for its file");
		return NULL;
	}

	/* The closure header may begin with `static`; we scan for `function`
	 * first, then walk left over an optional `static` keyword. */
	long fn_off = scan_find_keyword(&src, (size_t)line_off, "function");
	if (fn_off < 0) {
		free(src.data);
		*err = strdup("could not locate `function` keyword for inline closure");
		return NULL;
	}

	/* Find opening brace for the body. */
	long brace_open = -1;
	{
		scan_state_t st = SCAN_NORMAL;
		int paren_depth = 0;
		for (size_t i = (size_t)fn_off; i < src.len; i++) {
			char c = src.data[i];
			char n = (i + 1 < src.len) ? src.data[i + 1] : '\0';
			switch (st) {
				case SCAN_NORMAL:
					if (c == '\'') { st = SCAN_SQ_STRING; break; }
					if (c == '"')  { st = SCAN_DQ_STRING; break; }
					if (c == '/' && n == '/') { st = SCAN_LINE_COMMENT; i++; break; }
					if (c == '#'  && n != '[') { st = SCAN_LINE_COMMENT; break; }
					if (c == '/' && n == '*') { st = SCAN_BLOCK_COMMENT; i++; break; }
					if (c == '(') paren_depth++;
					if (c == ')') paren_depth--;
					if (c == '{' && paren_depth == 0) {
						brace_open = (long)i;
						goto found_brace;
					}
					break;
				case SCAN_SQ_STRING:
					if (c == '\\' && n != '\0') { i++; break; }
					if (c == '\'') st = SCAN_NORMAL;
					break;
				case SCAN_DQ_STRING:
					if (c == '\\' && n != '\0') { i++; break; }
					if (c == '"') st = SCAN_NORMAL;
					break;
				case SCAN_LINE_COMMENT:
					if (c == '\n') st = SCAN_NORMAL;
					break;
				case SCAN_BLOCK_COMMENT:
					if (c == '*' && n == '/') { st = SCAN_NORMAL; i++; }
					break;
			}
		}
found_brace:
		;
	}
	if (brace_open < 0) {
		free(src.data);
		*err = strdup("could not locate opening `{` for closure body");
		return NULL;
	}

	long body_end = scan_brace_match(&src, (size_t)brace_open);
	if (body_end < 0) {
		free(src.data);
		*err = strdup("unbalanced braces while extracting closure body");
		return NULL;
	}

	/* Walk left from fn_off over whitespace + optional `static` so we keep the
	 * canonical form intact for re-compilation in the worker. */
	long start = fn_off;
	while (start > line_off) {
		long s = start - 1;
		while (s > line_off && (src.data[s] == ' ' || src.data[s] == '\t')) s--;
		if (s >= line_off + 6 && memcmp(src.data + s - 5, "static", 6) == 0
			&& (s - 5 == 0 || !is_ident_byte(src.data[s - 6]))) {
			start = s - 5;
			continue;
		}
		break;
	}

	size_t slice_len = (size_t)(body_end - start);
	char *out = (char *)malloc(slice_len + 1);
	if (out == NULL) {
		free(src.data);
		*err = strdup("out of memory while extracting closure body");
		return NULL;
	}
	memcpy(out, src.data + start, slice_len);
	out[slice_len] = '\0';

	free(src.data);
	return out;
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Capture snapshot + rejection rules                                        */
/* ────────────────────────────────────────────────────────────────────────── */

static void throw_capture_error_msg(const char *msg)
{
	zend_throw_exception(parallax_capture_error_ce, msg, 0);
}

/* Walk the closure's static_variables HashTable; produce a VAL_ARR keyed by
 * each captured variable name. Rejects: IS_REFERENCE (use by-ref) and any
 * value rejected by px_zval_to_value (resource, nested closure, ...). */
static value_t *snapshot_captures(zend_function *fn)
{
	value_t *arr = value_arr(0);
	HashTable *uses = fn->op_array.static_variables;
	if (uses == NULL) {
		return arr;
	}

	zend_string *key;
	zval *slot;
	ZEND_HASH_FOREACH_STR_KEY_VAL(uses, key, slot) {
		if (key == NULL) {
			continue;
		}
		if (Z_TYPE_P(slot) == IS_REFERENCE) {
			value_free(arr);
			throw_capture_error_msg("by-reference `use(&$var)` cannot be captured into a parallax task");
			return NULL;
		}
		if (Z_TYPE_P(slot) == IS_UNDEF) {
			value_free(arr);
			char msg[256];
			snprintf(msg, sizeof(msg), "captured `use($%s)` was never assigned before go() — refusing to materialise UNDEF in the worker", ZSTR_VAL(key));
			throw_capture_error_msg(msg);
			return NULL;
		}
		value_t *child = px_zval_to_value(slot);
		if (child == NULL) {
			value_free(arr);
			return NULL;
		}
		value_arr_set_str(arr, ZSTR_VAL(key), ZSTR_LEN(key), child);
	} ZEND_HASH_FOREACH_END();

	return arr;
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Public entry — bridge.c::px_resolve_callable delegates here for inline.   */
/* ────────────────────────────────────────────────────────────────────────── */

int px_resolve_inline_closure(zval *closure_zv, px_callable_t *out)
{
	memset(out, 0, sizeof(*out));

	zend_object   *zobj = Z_OBJ_P(closure_zv);
	zend_function *fn   = zend_get_closure_method_def(zobj);
	if (fn == NULL || fn->type != ZEND_USER_FUNCTION) {
		throw_capture_error_msg("only user-defined inline closures can be captured");
		return -1;
	}

	/* Reject closures bound to $this — the worker has no equivalent
	 * instance and the spec mandates a CaptureError here. */
	zval *bound_this = zend_get_closure_this_ptr(closure_zv);
	if (bound_this != NULL && Z_TYPE_P(bound_this) != IS_UNDEF && Z_TYPE_P(bound_this) != IS_NULL) {
		throw_capture_error_msg("closure with bound $this cannot be captured; declare it `static` to detach the instance");
		return -1;
	}

	zend_string *fname = fn->op_array.filename;
	if (fname == NULL || ZSTR_LEN(fname) == 0) {
		throw_capture_error_msg("closure has no resolvable source file");
		return -1;
	}

	char *err = NULL;
	char *text = extract_closure_text(ZSTR_VAL(fname), (int)fn->op_array.line_start, &err);
	if (text == NULL) {
		char msg[512];
		snprintf(msg, sizeof(msg), "closure source extraction failed: %s", err ? err : "unknown");
		free(err);
		throw_capture_error_msg(msg);
		return -1;
	}

	value_t *captures = snapshot_captures(fn);
	if (captures == NULL) {
		free(text);
		return -1;
	}

	/* Build the worker-side factory:
	 *   <?php return static function (array $__cv) { extract($__cv); return [text]; };
	 *
	 * `extract` injects every captured variable into the factory scope; the
	 * inner closure's `use (...)` then snapshots them by value. Captures live
	 * for the duration of the factory call. */
	smart_str wrapper = {0};
	smart_str_appendl(&wrapper, "<?php return static function (array $__cv) { extract($__cv); return ", strlen("<?php return static function (array $__cv) { extract($__cv); return "));
	smart_str_appendl(&wrapper, text, strlen(text));
	smart_str_appendl(&wrapper, "; };", 4);
	smart_str_0(&wrapper);

	out->kind = PX_CALL_KIND_CLOSURE;
	out->closure_wrapper = (char *)malloc(ZSTR_LEN(wrapper.s) + 1);
	if (out->closure_wrapper == NULL) {
		smart_str_free(&wrapper);
		value_free(captures);
		free(text);
		throw_capture_error_msg("out of memory while preparing closure wrapper");
		return -1;
	}
	memcpy(out->closure_wrapper, ZSTR_VAL(wrapper.s), ZSTR_LEN(wrapper.s) + 1);
	smart_str_free(&wrapper);

	out->closure_captures = captures;
	free(text);
	return 0;
}
