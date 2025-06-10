(ns dev-test-jsonrpc
  (:require
   [clojure.java.io :as io]
   [cheshire.core :as json]
   [babashka.process :as p]
   [clojure.core.async :as async]))

(def describe-var-name "lotuc.babashka.pods/describe")

(defn start-stdio-jsonrpc
  [{:keys [proc-opts on-notification]
    :or {on-notification identity
         on-callback identity}}]
  (let [{:keys [in out] :as proc}
        (apply p/process proc-opts)
        writer (io/writer (:in proc))
        reader (io/reader (:out proc))
        out-ch (async/chan)
        pending-requests (atom {})
        send-request #(binding [*out* writer]
                        (println (json/generate-string %)))
        done (fn [id result]
               (when-some [p (some-> (swap-vals! pending-requests dissoc id)
                                     (first) (get id) (:p))]
                 (deliver p result)))
        partial (fn [id result]
                  (when-some [f (some-> (get @pending-requests id)
                                        (:on-partial))]
                    (f result)))]
    (async/go-loop []
      (if-some [line (binding [*in* reader]
                       (read-line))]
        (do
          (try (async/>! out-ch [:ok (json/parse-string line)])
               (catch Throwable _
                 (async/>! out-ch [:error line])))
          (recur))
        (async/close! out-ch)))
    (async/go-loop []
      (when-some [[t v] (async/<! out-ch)]
        (case t
          :ok
          (let [{:strs [id jsonrpc result method params]} v]
            (if id
              ;; assume not call from server.
              (done id result)
              (case method
                "lotuc.babashka.pods/notification"
                (let [{:strs [id type result err out data]} params]
                  (case type
                    "partial" (partial id result)
                    "stdout" (do (print out) (flush))
                    "stderr" (binding [*out* *err*] (print err) (flush))
                    "describe" (done describe-var-name data)
                    nil))
                (on-notification v))))
          (binding [*out* *err*] (println v)))
        (recur)))
    {:server-proc proc
     :send-request send-request
     :pending-requests pending-requests
     :out-ch out-ch}))

(defn invoke*
  ([rpc-server method params]
   (invoke* rpc-server method params nil))
  ([{:keys [send-request pending-requests]} method params {:keys [on-partial]}]
   (let [id (if (= method describe-var-name) describe-var-name (str (random-uuid)))
         p  (promise)]
     (when (get @pending-requests id)
       (throw (ex-info "duplicated id" {:id id})))
     (swap! pending-requests assoc id {:p p :on-partial on-partial})
     (send-request {"jsonrpc" "2.0"
                    "method"  method
                    "params"  params
                    "id"      id})
     [id p])))

(defn invoke
  ([rpc-server method]
   (invoke rpc-server method [] nil))
  ([rpc-server method params]
   (invoke rpc-server method params nil))
  ([{:keys [pending-requests] :as rpc-server} method params opts]
   (let [[id p] (invoke* rpc-server method params opts)]
     (future (try @p (finally (swap! pending-requests dissoc id)))))))

(defn shutdown [{:keys [server-proc send-request]}]
  (send-request {"jsonrpc" "2.0"
                 "method"  "lotuc.babashka.pods/shutdown"
                 "params"  []
                 "id"      (str (random-uuid))})
  (when (identical? ::timeout (deref server-proc 1000 ::timeout))
    (p/destroy-tree server-proc)))

(do (when-some [v (resolve 'rpc-server)] (when (bound? v) (shutdown @v)))
    (def rpc-server (start-stdio-jsonrpc
                     {:proc-opts ["./build/test_jsonrpc"]
                      :on-notification (fn [v] (prn :notification v))})))

(comment
  (async/<!! (:out-ch rpc-server))

  (deref (:server-proc rpc-server) 100 :timeout)

  @(invoke rpc-server "lotuc.babashka.pods/load-ns" "test-pod-defer")
  @(invoke rpc-server "lotuc.babashka.pods/describe")
  @(invoke rpc-server "test-pod/range_stream" [0 10]
           {:on-partial (fn [v] (prn :partial v))})
  @(invoke rpc-server "test-pod/echo" ["hello world"])
  @(invoke rpc-server "test-pod/add-sync" [1 2 3]))
