COMPOSE = docker compose
SERVICE = nginx

.PHONY: build up down restart logs ps shell test clean

build:
	$(COMPOSE) build

up:
	$(COMPOSE) up --build -d

down:
	$(COMPOSE) down

restart: down up

logs:
	$(COMPOSE) logs -f

ps:
	$(COMPOSE) ps

shell:
	$(COMPOSE) exec $(SERVICE) bash

test:
	curl -i http://localhost:8080/

clean:
	$(COMPOSE) down --rmi all --volumes --remove-orphans