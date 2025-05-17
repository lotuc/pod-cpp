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

(def print-handlers
  {:success (fn [e]
              (println [:success e]))
   :error   (fn [{:keys [ex-message ex-data]}]
              (println [:error ex-message ex-data]))})

(comment
  (def pod (reload-pod "test-pod-via-socket" {:transport :socket}))
  (def pod (reload-pod))
  (pods/unload-pod pod)

  (time (mapv deref (mapv (fn [_] (future (test-pod/sleep 100)))
                          (range 10))))
  (time (mapv deref (mapv (fn [_] (future (test-pod/async_sleep 100)))
                          (range 10))))

  (test-pod/echo)
  (test-pod/echo "hello world")

  (lotuc.babashka.pods/pendings)

  (test-pod/add-sync 1 2 3)

  ;; async
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

  (test-pod/mis_implementation "no-finish-message-sent")

  (require '[test-pod-defer])
  (test-pod-defer/add-sync 1 2 3)

  #_())

(comment
  (def n 10000)

  (do (def t4 (System/currentTimeMillis))
      (doseq [_ (range n)]
        #_{:clj-kondo/ignore [:unused-value]}
        (+ 42 24))
      (def t5 (System/currentTimeMillis)))
  (println)
  (println "[bb]    :" (- t5 t4))
  (doseq [[add-fn-name add-fn-sym]
          [["add-sync" 'test-pod/add-sync]
           ["add-async" 'test-pod/add-async]]]
    (reload-pod "perf-0")
    (let [add-fn @(resolve add-fn-sym)]
      (def t0 (System/currentTimeMillis))
      (doseq [_ (range n)]
        (add-fn 42 24))
      (def t1 (System/currentTimeMillis)))

    (reload-pod "perf-1" {:transport :socket})
    (let [add-fn @(resolve add-fn-sym)]
      (def t2 (System/currentTimeMillis))
      (doseq [_ (range n)]
        (add-fn 42 24))
      (def t3 (System/currentTimeMillis)))

    (println (str "--- " add-fn-name " ---"))
    (println "[stdio] :" (- t1 t0))
    (println "[socket]:" (- t3 t2)))

  (pods/unload-pod {:pod/id "perf-0"})
  (pods/unload-pod {:pod/id "perf-1"})

  ;; macOS 2.6 GHz 6-Core Intel Core i7
  ;; [bb]    : 4
  ;; --- add-sync ---
  ;; [stdio] : 1001
  ;; [socket]: 1930
  ;; --- add-async ---
  ;; [stdio] : 1512
  ;; [socket]: 2547

  #_())
