#!/bin/sh

set -e

usage() {
  echo "USAGE: setup.sh <archive>"
  echo "  <archive> is a .tar.gz artifact generated by a file writer build."
  exit 1
}

if [ -z $1 ]; then
  >&2 echo "Error: missing path to file writer artifact archive"
  >&2 echo ""
  usage
fi

FWARCHIVE=$1

echo "Preparing test..."
docker compose up -d

echo "Copying files..."
docker cp $FWARCHIVE filewriter:/home/jenkins/kafka-to-nexus.tar.gz
docker cp ../integration-tests filewriter:/home/jenkins/

echo "Installing dependencies..."
docker exec filewriter bash -c 'tar xzvf kafka-to-nexus.tar.gz'
docker exec filewriter bash -c 'scl enable rh-python38 -- python -m venv venv'
docker exec filewriter bash -c 'scl enable rh-python38 -- venv/bin/pip install -r integration-tests/requirements.txt'

echo "Creating Kafka topics..."
while read topic; do
  docker exec kafka bash -c "kafka-topics --bootstrap-server localhost:9092 --create --topic $topic"
done < topics.txt

echo "Preparation completed!"