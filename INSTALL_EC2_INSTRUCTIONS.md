# Install on EC2

## Docker

```
# Update the package index
sudo apt update

# Install Docker
sudo apt install docker.io -y

# Start the Docker service
sudo systemctl start docker

# Add your user to the Docker group
sudo usermod -aG docker $USER

```
Remember to log out and back in for the group changes to take effect.

## Docker compose

3 steps:

```
sudo curl -L "https://github.com/docker/compose/releases/download/v2.26.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
```

```
`sudo chmod +x /usr/local/bin/docker-compose`
```

```
`docker-compose --version`
```