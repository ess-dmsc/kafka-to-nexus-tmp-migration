#!/bin/sh

docker exec filewriter bash -c 'cd integration-tests; scl enable rh-python38 -- ~/venv/bin/pytest --writer-binary=../kafka-to-nexus --junitxml=IntegrationTestsOutput.xml'

result=$?

set -e

docker cp filewriter:/home/jenkins/integration-tests/IntegrationTestsOutput.xml .
docker cp filewriter:/home/jenkins/integration-tests/logs/. logs

exit $result
