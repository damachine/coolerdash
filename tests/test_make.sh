#!/bin/bash
# Test build and install using standard Makefile (for non-Arch systems)
# Usage: ./test_make.sh

set -e  # Exit on error

echo "🧹 Cleaning previous build..."
cd "$(dirname "$0")/.."
make clean

echo "🔨 Building with make..."
make

echo "📦 Installing with sudo make install..."
sudo make install

echo "🔄 Reloading systemd and restarting service..."
sudo systemctl daemon-reload
sudo systemctl restart coolerdash.service
sudo systemctl enable coolerdash.service

echo "✅ Done! Checking service status..."
systemctl status coolerdash.service --no-pager -l

echo ""
echo "📋 View logs: journalctl -u coolerdash.service -f"
