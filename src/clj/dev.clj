(ns dev)

(require '[clojure.java.io :as io])
(require '[bencode.core :as b])
(require '[babashka.process :as p])
(require '[cheshire.core :as json])
(require '[clojure.walk])
(require '[babashka.pods :as pods])

(def test-pod ["./build/test_pod"])

(defn string->stream
  ([s] (string->stream s "UTF-8"))
  ([s encoding]
   (-> s
       (.getBytes encoding)
       (java.io.ByteArrayInputStream.))))

(defn from-netstring [s]
  (b/read-bencode (java.io.PushbackInputStream. (string->stream s))))

(defn to-netstring [v]
  (-> (doto (java.io.ByteArrayOutputStream.)
        (b/write-bencode v))
      .toString))

(defn shut-dev-process []
  (when-some [v (resolve 'dev-process)]
    (when (bound? v) (p/destroy-tree @v))))

#_{:clj-kondo/ignore [:inline-def]}
(defn restart-dev-process []
  (def dev-process (apply p/process test-pod))

  (def !dev-echo
    (future
      (with-open [in (java.io.PushbackInputStream. (:out dev-process))]
        (loop []
          (prn (clojure.walk/postwalk
                (fn [v] (if (bytes? v) (String. v "UTF-8") v))
                (b/read-bencode in)))
          (recur)))))

  (def dev-out (io/writer (:in dev-process)))

  (defn w [d]
    (binding [*out* dev-out]
      (print (to-netstring d))
      (flush))))

(comment
  (def pod-id (pods/load-pod test-pod {:transport :socket}))
  (def pod-id (pods/load-pod test-pod))
  (pods/unload-pod pod-id)

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

  (to-netstring {:op "describe"})
  "d2:op8:describee"

  (shut-dev-process)
  (restart-dev-process)
  (w {:op "describe"})
  (w {:op "invoke" :id "42" :var "test_pod/echo"
      :args (json/generate-string ["hello world"])})

  (w {:op "invoke" :id "42" :var "test_pod/echo"
      :args (json/generate-string [])})

  (w {:op "invoke" :id "42" :var "test_pod/error"
      :args (json/generate-string [])})

  (to-netstring {:op "invoke" :id "42" :var "test_pod/echo"
                 :args (json/generate-string [])})
  "d4:args2:[]2:id2:422:op6:invoke3:var13:test_pod/echoe"

  (to-netstring {:op "invoke" :id "42" :var "test_pod/error"
                 :args (json/generate-string [])})
  "d4:args2:[]2:id2:422:op6:invoke3:var14:test_pod/errore"

  #_())
