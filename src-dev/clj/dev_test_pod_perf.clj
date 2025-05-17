(ns dev-test-pod-perf)

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
