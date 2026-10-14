/* Expand per-module core names before forming a GCC alias target. */
#pragma once

#define SYSCLIB_STRINGIFY_INNER(symbol) #symbol
#define SYSCLIB_STRINGIFY(symbol) SYSCLIB_STRINGIFY_INNER(symbol)
#define SYSCLIB_ALIAS_TARGET(symbol) \
	__attribute__((alias(SYSCLIB_STRINGIFY(symbol))))
