(ns dev-test-pod)

(require '[babashka.pods :as pods])

(defn pod-spec
  ([] ["./build/test_pod"])
  ([pod-id] ["./build/test_pod" pod-id])
  ([pod-id {:keys [max-concurrent]}]
   (if max-concurrent
     ["./build/test_pod" pod-id (str max-concurrent)]
     ["./build/test_pod" pod-id])))

(defn reload-pod
  ([pod-id opts]
   (pods/unload-pod {:pod/id pod-id})
   (pods/load-pod (pod-spec pod-id opts) opts))
  ([pod-id]
   (pods/unload-pod {:pod/id pod-id})
   (pods/load-pod (pod-spec pod-id)))
  ([]
   (pods/unload-pod {:pod/id "test-pod"})
   (pods/load-pod (pod-spec))))

(def print-handlers
  {:success (fn [e]
              (println [:success e]))
   :error   (fn [{:keys [ex-message ex-data]}]
              (println [:error ex-message ex-data]))})

(comment
  (def pod (reload-pod "test-pod-via-socket" {:transport :socket}))
  (def pod (reload-pod "test-pod-via-stdio" {:max-concurrent 3}))
  (pods/unload-pod pod)

  ;; async won't block the pod's read evaluation loop
  (time (mapv deref (mapv (fn [_] (future (test-pod/sleep 100)))
                          (range 10))))
  (time (mapv deref (mapv (fn [_] (future (test-pod/async_sleep 100)))
                          (range 10))))

  ;; I added a utility var lotuc.babashka.pods/pendings for tracking pending
  ;; vars (the unfinished async invokes).
  (let [start (System/currentTimeMillis)
        done #(println "finishes in:" (- (System/currentTimeMillis) start))]
    (future (test-pod/async_sleep 1000) (done))
    (future (test-pod/async_sleep 1000) (done))
    (future (test-pod/async_sleep 1000) (done))
    (future (test-pod/async_sleep 1000) (done))
    (Thread/sleep 100)
    (lotuc.babashka.pods/pendings))

  (test-pod/echo)
  (test-pod/echo "hello world")
  (test-pod/add-sync 1 2 3)

  ;; async function with multiple callback returns
  (babashka.pods/invoke
   (:pod/id pod)
   'test-pod/range_stream
   [0 10]
   {:handlers print-handlers})

  (babashka.pods/invoke
   (:pod/id pod)
   'test-pod/multi_threaded_test
   []
   {:handlers print-handlers})

  (resolve 'test-pod/do_twice)

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

  ;; If the var's implementation does not signal a finishing value, it will be
  ;; detected
  (test-pod/mis_implementation "no-finish-message-sent")

  ;; lazy loaded namespaces
  (require '[test-pod-defer])
  (test-pod-defer/add-sync 1 2 3)
  #_())
