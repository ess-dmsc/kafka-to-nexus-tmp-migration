from confluent_kafka import Producer, Consumer
import uuid


def create_producer():
    producer_config = {'bootstrap.servers': 'localhost:9092',
                       'message.max.bytes': '20000000'}
    producer = Producer(**producer_config)
    return producer


def send_writer_command(filepath, producer, topic="TEST_writerCommand", start_time=None):
    with open(filepath, "r") as cmd_file:
        data = cmd_file.read().replace('\n', '')
        if start_time is not None:
            data = data.replace('STARTTIME', start_time)
    producer.produce(topic, data)


def create_consumer():
    return Consumer({"bootstrap.servers": "localhost:9092", "group.id": uuid.uuid4(), 'default.topic.config': {'auto.offset.reset': 'latest'}})
