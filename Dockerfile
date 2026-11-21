FROM python:3.12-slim
WORKDIR /app
COPY relay_server/apex_relay_server.py /app/apex_relay_server.py
EXPOSE 7779
CMD ["python", "/app/apex_relay_server.py"]
