.PHONY: all build test integration-test docker-build up down

all: build

build:
	$(MAKE) -C apps/webserv
	$(MAKE) -C services/irc/prd

test: build
	$(MAKE) -C apps/webserv test
	$(MAKE) -C services/irc/tests test
	cd services/gateway && npm test
	cd services/gateway && npm run test:integration

integration-test: build
	$(MAKE) -C apps/webserv integration-test

docker-build:
	docker build -t irc-platform .

up:
	docker compose -f deploy/docker-compose.yml up --build

down:
	docker compose -f deploy/docker-compose.yml down
