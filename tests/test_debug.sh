#!/bin/bash
# Build and run coolerdash in debug mode (with AddressSanitizer)
# Usage: ./test_debug.sh [options]
# Options are passed to coolerdash (e.g., --verbose, --config /path/to/config.json)

set -e  # Exit on error

echo "🧹 Cleaning previous build..."
cd "$(dirname "$0")/.."
make clean

echo "🔨 Building debug version (AddressSanitizer enabled)..."
make debug

echo ""
echo "🐛 Running coolerdash in DEBUG mode..."
echo "   (Press Ctrl+C to stop)"
echo ""

# Run with all arguments passed to script
sudo bin/coolerdash "$@"
