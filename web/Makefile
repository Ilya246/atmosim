all: dist/index.html

serve: all # dev webserver environment
	cd dist
	python3 -m http.server

dist/index.html: dist/atmosim.js src/index.html src/main.js src/worker_integration.js
	cp src/* dist

dist/atmosim.js: ../atmosim.cpp dist/
	emcc -s INVOKE_RUN=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["callMain", "FS", "ENV"]' -o dist/atmosim.js ../atmosim.cpp -std=c++20

dist/:
	mkdir dist

clean:
	rm -rf dist

.PHONY: clean all serve
