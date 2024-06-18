FROM rippleci/clio_ci as build

COPY . .

RUN conan install . --build missing -if build

RUN conan build .  -if build && cmake --build build  --target install/strip

FROM debian:bullseye-slim as clio

COPY --from=build /opt/clio/bin/clio_server /opt/clio/bin/clio_server
RUN ls -s /opt/clio/bin/clio_server /usr/local/bin/clio_server
