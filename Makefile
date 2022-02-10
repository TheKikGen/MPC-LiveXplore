export UID=$(shell id -u)
export GID=$(shell id -g)

IMAGE=cfw-builder

ifeq (diff-firmwares,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(RUN_ARGS):;@:)
endif

ifeq (firmware-info,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(RUN_ARGS):;@:)
endif

ifeq (build-cfw,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(RUN_ARGS):;@:)
endif

all: docker-image

.PHONY: docker-image
docker-image:
	@docker image rm -f ${IMAGE} >/dev/null
	DOCKER_BUILDKIT=1 docker build -t ${IMAGE} .

.PHONY: shell
shell: docker-image
	docker run --user="${UID}:${GID}" --rm -v $$(pwd):/data -it ${IMAGE} /bin/bash

.PHONY: firmware-info
firmware-info:
	docker run --user="${UID}:${GID}" --rm -v $$(pwd):/data -it ${IMAGE} /bin/bash -c "./cfw-builder/firmware-info.sh info $(RUN_ARGS)"

.PHONY: diff-firmwares
diff-firmwares:
	docker run --user="${UID}:${GID}" --rm -v $$(pwd):/data -it ${IMAGE} /bin/bash -c "./cfw-builder/diff-firmwares.sh $(RUN_ARGS)"

.PHONY: build-cfw
build-cfw:
	docker run --user="${UID}:${GID}" --rm -v $$(pwd):/data -it ${IMAGE} /bin/bash -c "./cfw-builder/build-cfw.sh $(RUN_ARGS)"