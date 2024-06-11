FROM rippleci/clio_ci

RUN conan install . --build missing -if build

RUN conan build -if build
