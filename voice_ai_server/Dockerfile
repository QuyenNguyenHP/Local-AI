# CUDA runtime matches the GPU PyTorch wheel in requirements-gpu.txt.
FROM nvidia/cuda:12.8.1-cudnn-runtime-ubuntu24.04

ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PATH=/opt/venv/bin:$PATH

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 python3-venv ffmpeg libsndfile1 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements-gpu.txt requirements.txt ./
RUN python3 -m venv /opt/venv \
    && pip install --no-cache-dir -r requirements-gpu.txt \
    && pip install --no-cache-dir -r requirements.txt

COPY . ./
EXPOSE 8000
CMD ["python", "run.py"]
