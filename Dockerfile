FROM rippleci/clio_ci

COPY . .

RUN conan install . --build missing -if build

RUN conan build .  -if build
