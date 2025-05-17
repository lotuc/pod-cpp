# babashka pods - cpp

For the ease of building [babashka pods](https://github.com/babashka/pods) with
cpp.

## Walkthrough

Build the test pod (the implementations are in
[src-dev/cpp/test_pod.cpp](src-dev/cpp/test_pod.cpp)).

```
brew install nlohmann-json asio

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build -S .
cmake --build build
```

Load & run (start bababshka repl with `bb`, more examples in
[src-dev/clj/dev.clj](src/clj/dev.clj))

```clojure
(require '[babashka.pods :as pods])

;; the built pod file
(def test-pod ["./build/test_pod"])

;; load pod
(def pod (pods/load-pod test-pod))

;; sync call
(test-pod/add-sync 1 2 3)

;; async call with multiple callback returns
(babashka.pods/invoke
 (:pod/id pod)
 'test-pod/range_stream
 [0 10]
 {:handlers
  {:success
    (fn [e]
      (println [:success e]))
   :error
    (fn [{:keys [ex-message ex-data]}]
      (println [:error ex-message ex-data]))}})

;; lazy loaded namespaces
(resolve 'test-pod-defer/add-sync) ;; => nil
(require '[test-pod-defer])
(test-pod-defer/add-sync 1 2 3)

;; unload pod
(pods/unload-pod pod)
```
