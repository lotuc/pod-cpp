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

#_{:clj-kondo/ignore [:inline-def]}
(defn _init []
  (when-some [v (resolve 'a)] (when (bound? v) (p/destroy-tree @v)))

  (def a (apply p/process test-pod))

  (def !f
    (future
      (with-open [in (java.io.PushbackInputStream. (:out a))]
        (loop []
          (prn (clojure.walk/postwalk
                (fn [v] (if (bytes? v) (String. v "UTF-8") v))
                (b/read-bencode in)))
          (recur)))))

  (def out (io/writer (:in a)))

  (defn w [d]
    (binding [*out* out]
      (print (to-netstring d))
      (flush))))

(comment
  (def pod-id (pods/load-pod test-pod {:transport :socket}))
  (def pod-id (pods/load-pod test-pod))
  (pods/unload-pod pod-id)

  (pod.test-pod/add-sync 1 2 3)

  (echo/echo 42)
  (echo/echo "hello world")
  (echo/echo ["hello" "world"])

  (doseq [i (range 100)]
    (future (println (echo/echo i))))

  (to-netstring {:op "describe"})
  "d2:op8:describee"

  (_init)
  (w {:op "describe"})
  (w {:op "invoke"
      :id "4"
      :var "echo/echo"
      :args (json/generate-string ["hello"])})

  (w {:op "invoke"
      :id "4"
      :var "echo/echo"
      :args (json/generate-string [])})

  #_())
