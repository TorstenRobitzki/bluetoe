# Container engine to be used (typically: podman|docker)
CONTAINER_ENGINE := docker

# Name of the container image
CONTAINER_IMAGE ?= bluetoe_alpine

# Mount point where the quark-derived project will exist within the container
CONTAINER_TARGET := /work

COMMAND ?= mkdir -p ../build && cd ../build && cmake -GNinja -DARM_GCC_TOOL_PATH=/usr/local/gcc-arm-none-eabi-10.3-2021.10/ -DNRF5_SDK_ROOT=/opt/nrf5_sdk/ -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake .. && ninja

.PHONY: all
all: container-build

.PHONY: clean
clean:
	rm -rf ../build

.PHONY: container-build-image
container-build-image:
	@image_sha=`$(CONTAINER_ENGINE) images -q $(CONTAINER_IMAGE)`;\
	if [ -z "$$image_sha" ];\
    then\
    echo "Building $(CONTAINER_IMAGE). This will take a few minutes...";\
	$(CONTAINER_ENGINE) build --rm --tag $(CONTAINER_IMAGE) .;\
	else\
    echo "Using existing image $(CONTAINER_IMAGE)-> $$image_sha ";\
	fi

# If there is no local image with the required image name, the recipe tries to pull
# the image from a container registry.
.PHONY: container-pull-image
container-pull-image:
	@image_sha=`$(CONTAINER_ENGINE) images -q $(CONTAINER_IMAGE)`;\
	if [ -z "$$image_sha" ];\
    then\
    echo "Did not find a local container image with name:$(CONTAINER_IMAGE). Fetching one from the container registry...";\
	$(CONTAINER_ENGINE) pull $(CONTAINER_IMAGE);\
	else\
    echo "Using existing local image $(CONTAINER_IMAGE)-> $$image_sha ";\
	fi

.PHONY: container-shell
container-shell: container-build-image
	@$(CONTAINER_ENGINE) run --rm -ti \
	--user=$(shell id -u):$(shell id -g) \
	--mount type="bind,src=$(PWD)/../..,target=$(CONTAINER_TARGET)" \
	--workdir="$(CONTAINER_TARGET)" $(CONTAINER_IMAGE)

.PHONY: container-build
container-build: container-build-image
	$(CONTAINER_ENGINE) run --rm -ti \
	--user=$(shell id -u):$(shell id -g) \
	--mount type="bind,src=$(PWD)/../..,target=$(CONTAINER_TARGET)" \
	--workdir="$(CONTAINER_TARGET)/examples/docker" \
	$(CONTAINER_IMAGE) sh -c "$(COMMAND)"

.PHONY: container-delete
container-delete:
	@$(CONTAINER_ENGINE) rmi $(CONTAINER_IMAGE)

.PHONY: help
help:
	@echo "Supported targets:"
	@echo "  all                     Executes the ./buildAll.sh script inside the container"
	@echo "  container-build-image   Builds the $(CONTAINER_IMAGE) image based on the Dockerfile"
	@echo "  container-pull-image    Fetches the $(CONTAINER_IMAGE) image from the container registry"
	@echo "  container-shell         Opens a shell in a temporary container"
	@echo "  container-build         Executes COMMAND inside a temporary container"
	@echo "  container-delete        Deletes the $(CONTAINER_IMAGE) image"
	@echo "  help                    You're looking at it"
