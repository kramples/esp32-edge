"""AWS Lambda handler: ingest ESP32 telemetry from AWS IoT Core into DynamoDB.

Invoked asynchronously by an AWS IoT rule subscribed to
``esp32-edge/+/telemetry``. The rule passes the device's JSON payload as the
event. Valid records are written to the DynamoDB table named by the
``TABLE_NAME`` environment variable (partition key ``deviceId``, sort key
``timestamp``).

All log lines are structured JSON for easy CloudWatch Logs Insights queries.
"""

import json
import logging
import os
from decimal import Decimal

import boto3

logger = logging.getLogger()
logger.setLevel(logging.INFO)

TABLE_NAME = os.environ.get("TABLE_NAME", "esp32-edge-telemetry")

_dynamodb = boto3.resource("dynamodb")
_table = _dynamodb.Table(TABLE_NAME)

REQUIRED_FIELDS = ("deviceId", "timestamp")
METRIC_FIELDS = ("temperature_c", "humidity_pct", "pm1_0", "pm2_5", "pm10")


def _log(level: int, message: str, **fields) -> None:
    logger.log(level, json.dumps({"message": message, **fields}))


def _validate(event: dict) -> list:
    """Return a list of validation problems (empty when the event is valid)."""
    problems = [f"missing required field: {f}" for f in REQUIRED_FIELDS if f not in event]
    if not any(f in event for f in METRIC_FIELDS):
        problems.append("payload contains no metric fields")
    return problems


def lambda_handler(event, context):
    request_id = getattr(context, "aws_request_id", "local")

    try:
        _log(logging.INFO, "telemetry received", requestId=request_id, event=event)

        problems = _validate(event)
        if problems:
            _log(logging.WARNING, "payload rejected", requestId=request_id, problems=problems)
            return {"statusCode": 400, "body": {"errors": problems}}

        # DynamoDB rejects Python floats — round-trip through Decimal.
        item = json.loads(json.dumps(event), parse_float=Decimal)
        item["deviceId"] = str(item["deviceId"])
        item["timestamp"] = int(item["timestamp"])

        _table.put_item(Item=item)

        _log(
            logging.INFO,
            "telemetry stored",
            requestId=request_id,
            table=TABLE_NAME,
            deviceId=item["deviceId"],
            timestamp=item["timestamp"],
        )
        return {"statusCode": 200, "body": {"stored": True}}

    except Exception:
        _log(logging.ERROR, "ingestion failed", requestId=request_id)
        logger.exception("unhandled error")
        raise  # let Lambda mark the invocation failed so IoT retry/DLQ applies
