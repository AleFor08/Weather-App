FROM yhirose4dockerhub/ubuntu-builder AS builder
WORKDIR /build
COPY include/cpp-httplib-0.30.1/httplib.h .
COPY main.cpp .
# nlohmann single-header json
COPY include/json-3.12.0/single_include/nlohmann/json.hpp nlohmann/json.hpp
RUN g++ -std=c++23 -static -o server -O2 -I. main.cpp && strip server

FROM scratch
COPY --from=builder /build/server /server
COPY include/cpp-httplib-0.30.1/docker/html/index.html /html/index.html
EXPOSE 80

ENTRYPOINT ["/server"]
CMD ["--host", "0.0.0.0", "--port", "80", "--mount", "/:./html"]
