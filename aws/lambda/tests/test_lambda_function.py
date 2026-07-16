"""Local validation for lambda_function.py (run with: pytest aws/lambda/tests)."""

import sys
from decimal import Decimal
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

with patch("boto3.resource"):
    import lambda_function


VALID_EVENT = {
    "deviceId": "esp32-edge-01",
    "timestamp": 1752537600,
    "temperature_c": 24.5,
    "humidity_pct": 41.0,
    "pm1_0": 3,
    "pm2_5": 7,
    "pm10": 9,
}


@pytest.fixture
def table():
    mock_table = MagicMock()
    with patch.object(lambda_function, "_table", mock_table):
        yield mock_table


def test_valid_event_is_stored(table):
    result = lambda_function.lambda_handler(VALID_EVENT, None)

    assert result["statusCode"] == 200
    item = table.put_item.call_args.kwargs["Item"]
    assert item["deviceId"] == "esp32-edge-01"
    assert item["timestamp"] == 1752537600
    # Floats must arrive at DynamoDB as Decimal
    assert isinstance(item["temperature_c"], Decimal)


def test_missing_required_field_is_rejected(table):
    event = dict(VALID_EVENT)
    del event["deviceId"]

    result = lambda_function.lambda_handler(event, None)

    assert result["statusCode"] == 400
    table.put_item.assert_not_called()


def test_payload_without_metrics_is_rejected(table):
    event = {"deviceId": "esp32-edge-01", "timestamp": 1752537600}

    result = lambda_function.lambda_handler(event, None)

    assert result["statusCode"] == 400
    table.put_item.assert_not_called()


def test_dynamodb_error_propagates_for_retry(table):
    table.put_item.side_effect = RuntimeError("throttled")

    with pytest.raises(RuntimeError):
        lambda_function.lambda_handler(VALID_EVENT, None)
