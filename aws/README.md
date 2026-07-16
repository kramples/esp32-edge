# AWS Cloud Setup

Serverless ingestion pipeline: **IoT Core rule → Lambda → DynamoDB**, monitored via CloudWatch.
Replace `REGION` and `ACCOUNT_ID` in the policy templates before use.

## 1. DynamoDB table

```sh
aws dynamodb create-table \
  --table-name esp32-edge-telemetry \
  --attribute-definitions AttributeName=deviceId,AttributeType=S AttributeName=timestamp,AttributeType=N \
  --key-schema AttributeName=deviceId,KeyType=HASH AttributeName=timestamp,KeyType=RANGE \
  --billing-mode PAY_PER_REQUEST
```

## 2. IoT Thing, certificates & policy

1. IoT Core → **Things → Create thing** (name must match `THING_NAME` in `secrets.h`, e.g. `esp32-edge-01`).
2. Auto-generate a certificate; download the device cert, private key, and Amazon Root CA 1 into `firmware/include/secrets.h`.
3. Create a policy from [policy_templates/iot_device_policy.json](policy_templates/iot_device_policy.json) and attach it to the certificate. It is least-privilege: the device may only connect as its own Thing name and publish to `esp32-edge/<thing>/telemetry`.

## 3. Lambda function

1. Create function `esp32-edge-ingest` (Python 3.12) from [lambda/lambda_function.py](lambda/lambda_function.py).
2. Set environment variable `TABLE_NAME=esp32-edge-telemetry`.
3. Attach [policy_templates/lambda_execution_role_policy.json](policy_templates/lambda_execution_role_policy.json) to its execution role (DynamoDB `PutItem` on the one table + CloudWatch Logs only).

Local validation:

```sh
pip install pytest boto3
pytest aws/lambda/tests
```

## 4. IoT rule

Message routing → Rules → Create rule `esp32_edge_telemetry`:

```sql
SELECT * FROM 'esp32-edge/+/telemetry'
```

Action: invoke the `esp32-edge-ingest` Lambda. Let the console add the `lambda:InvokeFunction` resource permission for `iot.amazonaws.com`, or add it yourself:

```sh
aws lambda add-permission \
  --function-name esp32-edge-ingest \
  --statement-id iot-rule-invoke \
  --action lambda:InvokeFunction \
  --principal iot.amazonaws.com \
  --source-arn arn:aws:iot:REGION:ACCOUNT_ID:rule/esp32_edge_telemetry
```

## 5. CloudWatch log retention (required)

Never leave retention at "Never Expire":

```sh
aws logs put-retention-policy \
  --log-group-name /aws/lambda/esp32-edge-ingest \
  --retention-in-days 14
```

## 6. Verify ingestion

1. Power the device; watch the serial monitor for `[mqtt] published`.
2. IoT Core → MQTT test client → subscribe to `esp32-edge/#`.
3. Check CloudWatch Logs Insights (`/aws/lambda/esp32-edge-ingest`) for `"message": "telemetry stored"`.
4. `aws dynamodb scan --table-name esp32-edge-telemetry --max-items 5`
