FROM python:3.12-slim
WORKDIR /app
COPY relay_server/apex_relay_server.py /app/apex_relay_server.py
COPY relay_server/web_game.html /app/web_game.html
COPY relay_server/assets /app/assets
EXPOSE 7779
CMD ["python", "/app/apex_relay_server.py"]
