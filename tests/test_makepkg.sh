#!/bin/bash
# Quick build and install script for Arch Linux testing
# Usage: ./build-test.sh

set -e  # Exit on error

echo "🧹 Cleaning previous build..."
make clean

echo "📝 Updating .SRCINFO..."
makepkg --printsrcinfo > .SRCINFO

echo "📦 Building and installing package..."
makepkg -fsi --noconfirm

echo "🔄 Reloading systemd and restarting service..."
sudo systemctl daemon-reload
sudo systemctl restart coolerdash.service
sudo systemctl enable coolerdash.service

echo "✅ Done! Checking service status..."
systemctl status coolerdash.service --no-pager -l

echo ""
echo "📋 View logs: journalctl -u coolerdash.service -f"
