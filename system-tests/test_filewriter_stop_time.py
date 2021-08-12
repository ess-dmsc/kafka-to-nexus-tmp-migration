from helpers.nexushelpers import OpenNexusFile
from helpers.kafkahelpers import (
    create_producer,
    publish_f142_message,
)
from time import sleep
from datetime import datetime, timedelta
import json
from streaming_data_types.fbschemas.logdata_f142.AlarmStatus import AlarmStatus
from streaming_data_types.fbschemas.logdata_f142.AlarmSeverity import AlarmSeverity
import pytest
from file_writer_control.WriteJob import WriteJob
from helpers.writer import (
    wait_start_job,
    wait_writers_available,
    wait_no_working_writers,
)


def test_start_and_stop_time_are_in_the_past(
    writer_channel, worker_pool, kafka_address
):
    wait_writers_available(writer_channel, nr_of=1, timeout=10)
    producer = create_producer()

    data_topics = ["TEST_historicalData1", "TEST_historicalData2"]

    start_time = datetime(year=2019, month=6, day=12, hour=11, minute=1, second=35)
    stop_time = start_time + timedelta(seconds=200)
    step_time = timedelta(seconds=1)
    alarm_change_time_1 = start_time + timedelta(seconds=50)
    alarm_change_time_2 = start_time + timedelta(seconds=60)

    # Publish some data with timestamps in the past(these are from 2019 - 06 - 12)
    for data_topic in data_topics:
        current_time = start_time
        while current_time < stop_time:
            if current_time == alarm_change_time_1:
                # EPICS alarm goes into HIGH state
                publish_f142_message(
                    producer,
                    data_topic,
                    current_time,
                    alarm_status=AlarmStatus.HIGH,
                    alarm_severity=AlarmSeverity.MAJOR,
                )
            elif current_time == alarm_change_time_2:
                # EPICS alarm returns to NO_ALARM
                publish_f142_message(
                    producer,
                    data_topic,
                    current_time,
                    alarm_status=AlarmStatus.NO_ALARM,
                    alarm_severity=AlarmSeverity.NO_ALARM,
                )
            else:
                publish_f142_message(producer, data_topic, current_time)
            current_time += step_time

    file_name = "output_file_of_historical_data.nxs"
    file_start_time = start_time + timedelta(seconds=2)
    file_stop_time = start_time + timedelta(seconds=148)
    with open("commands/nexus_structure_historical.json", "r") as f:
        structure = f.read()
    write_job = WriteJob(
        nexus_structure=structure,
        file_name=file_name,
        broker=kafka_address,
        start_time=file_start_time,
        stop_time=file_stop_time,
    )
    wait_start_job(worker_pool, write_job, timeout=20)

    wait_no_working_writers(writer_channel, timeout=30)

    # The command also includes a stream for topic TEST_emptyTopic which exists but has no data in it, the
    # file writer should recognise there is no data in that topic and close the corresponding streamer without problem.
    file_path = f"output-files/{file_name}"
    with OpenNexusFile(file_path) as file:
        # Expect to have recorded one value per ms between the start and stop time
        # +3 due to writing one message before start and one message after stop
        expected_elements = (file_stop_time - file_start_time) // step_time + 3
        assert file["entry/historical_data_1/time"].len() == (
            expected_elements
        ), "Expected there to be one message per millisecond recorded between specified start and stop time"
        assert file["entry/historical_data_2/time"].len() == (
            expected_elements
        ), "Expected there to be one message per millisecond recorded between specified start and stop time"

        # EPICS alarms
        assert (
            file["entry/historical_data_1/alarm_status"].len() == 2
        ), "Expected there to have record two changes in EPICS alarm status"
        assert (
            file["entry/historical_data_1/alarm_severity"].len() == 2
        ), "Expected there to have record two changes in EPICS alarm status"
        # First alarm change
        assert file["entry/historical_data_1/alarm_status"][0] == b"HIGH"
        assert file["entry/historical_data_1/alarm_severity"][0] == b"MAJOR"
        assert file["entry/historical_data_1/alarm_time"][0] == int(
            alarm_change_time_1.timestamp() * 1e9
        )  # ns
        # Second alarm change
        assert file["entry/historical_data_1/alarm_status"][1] == b"NO_ALARM"
        assert file["entry/historical_data_1/alarm_severity"][1] == b"NO_ALARM"
        assert file["entry/historical_data_1/alarm_time"][1] == int(
            alarm_change_time_2.timestamp() * 1e9
        )  # ns
