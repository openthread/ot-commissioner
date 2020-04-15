# build with docker: 
# docker build -t builder .

FROM ubuntu:19.10

# install build tools and libreadline
RUN apt update -y && apt install -y python3 python3-pip cmake make gcc g++ libreadline-dev
RUN pip3 install conan

# copy source
COPY . /tmp/.
WORKDIR /tmp/

# make sure build dir is there and is clean
RUN mkdir build || true && rm -rf build/* || true

# install conan dependencies
RUN cd build && conan remote add gocarlos "https://api.bintray.com/conan/gocarlos/public-conan" && conan install .. --build missing

# build using cmake and find package
RUN cd build && cmake .. -DOT_COMM_USE_VENDORED_LIBS=OFF && cmake --build . -j 
