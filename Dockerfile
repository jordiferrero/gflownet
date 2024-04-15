FROM python:3.10-slim

USER root
# Install git
RUN apt-get update && \
    apt-get install -y git && \
    rm -rf /var/lib/apt/lists/*

RUN pip install jupyterlab
WORKDIR /code

COPY . /code

RUN pip install -e /code --find-links https://data.pyg.org/whl/torch-2.1.2+cpu.html

# Expose Jupyter port
EXPOSE 8888

CMD ["jupyter", "lab", "--port=8888", "--no-browser", "--ip=0.0.0.0", "--allow-root" ,"--NotebookApp.token=''",  "--NotebookApp.password=''"]
