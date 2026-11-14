#!/bin/zsh
set -e
cd "$(dirname "$0")/../.."
python3 ports/desktop/build.py
open -n "artifacts/desktop/GVIEW Travel Simulator.app" --args --replace-running
