#!/bin/bash
# Uninstall coolerdash using Makefile
# Usage: ./test_uninstall.sh

set -e  # Exit on error

echo "🛑 Stopping and disabling service..."
sudo systemctl stop coolerdash.service 2>/dev/null || true
sudo systemctl disable coolerdash.service 2>/dev/null || true

echo "🗑️  Uninstalling with make uninstall..."
cd "$(dirname "$0")/.."
sudo make uninstall

echo "🔄 Reloading systemd..."
sudo systemctl daemon-reload

echo "✅ Uninstall complete!"
echo ""
echo "📋 Verify removal:"
echo "  - Binary: ls /usr/local/bin/coolerdash"
echo "  - Config: ls /etc/coolerdash/"
echo "  - Service: systemctl status coolerdash.service"
