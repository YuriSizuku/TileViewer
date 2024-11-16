PROJECT_HOME=$(pwd)
if [ -z "${DOCKER_ARCH}" ]; then
DOCKER_ARCH=x86_64
fi
DOKCER_FILE=script/Dockerfile
DOCKER_IMAGE=tileviewer_${DOCKER_ARCH}
DOCKER_RUN=${DOCKER_IMAGE}_run
DOCKER_HOME=/project

if [ -z "$USE_BUILDX" ]; then
    docker_build="docker build"
else
    docker_build="docker buildx build"
fi
echo "## DOCKER_ARCH=${DOCKER_ARCH}"
echo "## docker_build=${docker_build}"

if [ -z "$(docker images | grep ${DOCKER_IMAGE})" ]; then 
    $docker_build ./ --platform linux/${DOCKER_ARCH} \
        --build-arg DOCKER_ARCH=${DOCKER_ARCH} \
        -f ${DOKCER_FILE} -t ${DOCKER_IMAGE} 
fi

if [ -z "$(docker ps | grep ${DOCKER_RUN})" ]; then
    docker run -d -it --rm -v ${PROJECT_HOME}:${DOCKER_HOME} \
        --name ${DOCKER_RUN} ${DOCKER_IMAGE}
fi

docker exec -i -e BUILD_TYPE=$BUILD_TYPE  -e BUILD_DIR=$BUILD_DIR \
     ${DOCKER_RUN} bash -c "cd ${DOCKER_HOME}; bash script/build_linux.sh"
