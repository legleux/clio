FROM rippleci/clio_ci

COPY . .

RUN conan install . -o static=True --build missing -if build

RUN conan build . -if build
RUN cmake --build build/Release  --target install/strip

# FROM debian:bullseye-slim as clio

# COPY --from=build /opt/clio/bin/clio_server /opt/clio/bin/clio_server
# RUN ln -s /opt/clio/bin/clio_server /usr/local/bin/clio_server
