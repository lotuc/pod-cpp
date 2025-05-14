(ns dev-test-pod)

(require '[babashka.pods :as pods])

(def test-pod ["./build/test_pod"])

(comment
  (def pod-id (pods/load-pod test-pod {:transport :socket}))
  (def pod-id (pods/load-pod test-pod))
  (pods/unload-pod pod-id)

  (test-pod/add-sync 1 2 3)

  (test-pod/error "hello")
  (test-pod/echo 42)
  (test-pod/echo "hello world")
  (test-pod/echo ["hello" "world"])

  (test-pod/return_nil)
  (test-pod/print "hello")
  (test-pod/print "hello" "world")
  (test-pod/print_err "hello")
  (test-pod/print_err "hello" "world")
  (test-pod/do-twice (println "hello"))
  (test-pod/fn-call (fn [x] (+ x 42)) 24)

  (require '[test-pod-defer])
  (test-pod-defer/add-sync 1 2 3)

  #_())
