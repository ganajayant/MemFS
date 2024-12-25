FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && apt-get upgrade -y && apt-get install -y bc vim g++ gcc make curl git && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
