<?php

declare(strict_types=1);

/**
 * Bootstrap loaded inside each parallax worker thread. Pulls in the Composer
 * autoloader so the Fixtures namespace is resolvable.
 */

require __DIR__ . '/../../vendor/autoload.php';
