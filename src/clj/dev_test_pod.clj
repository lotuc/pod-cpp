(ns dev-test-pod)

(require '[babashka.pods :as pods])

(defn pod-spec
  ([] ["./build/test_pod"])
  ([pod-id] ["./build/test_pod" pod-id]))

(defn reload-pod
  ([pod-id opts]
   (pods/unload-pod {:pod/id pod-id})
   (pods/load-pod (pod-spec pod-id) opts))
  ([pod-id]
   (pods/unload-pod {:pod/id pod-id})
   (pods/load-pod (pod-spec pod-id)))
  ([]
   (pods/unload-pod {:pod/id "test-pod"})
   (pods/load-pod (pod-spec))))

(comment
  (def pod (reload-pod "test-pod-via-socket" {:transport :socket}))
  (def pod (reload-pod))
  (pods/unload-pod pod)

  (test-pod/add-sync 1 2 3)

  ;; async
  (babashka.pods/invoke
   (:pod/id pod)
   'test-pod/range_stream
   [0 10]
   {:handlers
    {:success (fn [e]
                (println [:success e]))
     :error   (fn [{:keys [ex-message ex-data]}]
                (println [:error ex-message ex-data]))}})

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

  (require '[test-pod-defer])
  (test-pod-defer/add-sync 1 2 3)

  #_())

(comment
  (def n 10000)
  (reload-pod "perf-0")
  (do (def t0 (System/currentTimeMillis))
      (doseq [_ (range n)]
        (test-pod/add-sync 42 24))
      (def t1 (System/currentTimeMillis)))

  (reload-pod "perf-1" {:transport :socket})
  (do
    (def t2 (System/currentTimeMillis))
    (doseq [_ (range n)]
      (test-pod/add-sync 42 24))
    (def t3 (System/currentTimeMillis)))

  (do (def t4 (System/currentTimeMillis))
      (doseq [_ (range n)]
        (+ 42 24))
      (def t5 (System/currentTimeMillis)))

  (do (println "")
      (println "[stdio] :" (- t1 t0))
      (println "[socket]:" (- t3 t2))
      (println "[bb]    :" (- t5 t4)))

  (pods/unload-pod {:pod/id "perf-0"})
  (pods/unload-pod {:pod/id "perf-1"})

  #_())
