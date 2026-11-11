#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/mock_server.py & pid=$!
trap 'kill "$pid" 2>/dev/null || : ' EXIT
sleep .25
export EASYDAB_TEST_PASSWORD='dummy'
for spec in 'status:CONNECTED' 'amplitude:122' 'config:10.26.76.1:8888'; do
  cmd=${spec%%:*}; match=${spec#*:}
  ./easydabctl --host 127.0.0.1 --password-env EASYDAB_TEST_PASSWORD "$cmd" | grep -q "$match"
done
./easydabctl --host 127.0.0.1 --password-env EASYDAB_TEST_PASSWORD --json status | grep -q '"buffer_percent":71.8'
if ./easydabctl --host 127.0.0.1 --password-env EASYDAB_TEST_PASSWORD rf off 2>/dev/null; then
 echo 'Write safety check FAILED';exit 1
fi
./easydabctl --host 127.0.0.1 --password-env EASYDAB_TEST_PASSWORD --yes eti reconnect | grep -q CONNECTED
./easydabctl --host 127.0.0.1 --password-env EASYDAB_TEST_PASSWORD --yes amplitude set 122 | grep -q '122 / 255'
echo 'Mock tests PASS'
