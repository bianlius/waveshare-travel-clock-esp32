#!/bin/zsh
set -e
cd "$(dirname "$0")/../.."
python3 ports/desktop/build.py --passport
open -n "artifacts/desktop-passport/GVIEW Travel Passport Simulator.app" --args --replace-running
