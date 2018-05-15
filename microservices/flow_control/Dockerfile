FROM python:3-slim

WORKDIR /opt/app
COPY requirements.txt main.py adc.py .
RUN apt update && apt upgrade -y && pip install -r requirements.txt

ENTRYPOINT python main.py
