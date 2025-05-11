# babashka pods - cpp

For the ease of building [babashka pods](https://github.com/babashka/pods) with
cpp.

## Walkthrough

Build

```
brew install nlohmann-json asio

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build -S .
cmake --build build
```

Load & run (start bababshka repl with `bb`)

```clojure
(require '[babashka.pods :as pods])

;; the built pod file
(def test-pod ["./build/test_pod"])

;; load pod
(def pod-id (pods/load-pod test-pod))
(pods/unload-pod pod-id)

;; the functions
(pod.test-pod/add-sync 1 2 3)

(test_pod/error "hello")
(test_pod/echo 42)
(test_pod/echo "hello world")
(test_pod/echo ["hello" "world"])

(test_pod/return_nil)
(test_pod/print "hello")
(test_pod/print "hello" "world")
(test_pod/print_err "hello")
(test_pod/print_err "hello" "world")
(test_pod/do-twice (println "hello"))
(test_pod/fn-call (fn [x] (+ x 42)) 24)

(pods/unload-pod pod-id)
```

Related files
- [src-dev/cpp/test_pod.cpp](src-dev/cpp/test_pod.cpp)
- [src/clj/dev.clj](src/clj/dev.clj)
