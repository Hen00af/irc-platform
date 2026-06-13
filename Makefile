.PHONY: build shell rebuild clean

build:
	docker compose build

shell:
	docker compose run --rm webserv bash

rebuild:
	docker compose build
	docker compose run --rm webserv bash

clean:
	docker compose down --rmi all --volumes --remove-orphans