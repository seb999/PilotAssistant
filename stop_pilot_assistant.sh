#!/bin/bash

# Stop the Pilot Assistant systemd service

echo "Stopping Pilot Assistant service..."
sudo systemctl stop pilot-assistant.service

# Check if it's still running
if systemctl is-active --quiet pilot-assistant.service; then
    echo "ERROR: Service is still running"
    exit 1
else
    echo "Service stopped successfully"
fi

# Show current status
echo ""
echo "Service status:"
sudo systemctl status pilot-assistant.service --no-pager
