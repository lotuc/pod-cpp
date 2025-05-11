(ns dev)

(require '[clojure.java.io :as io])
(require '[bencode.core :as b])
(require '[babashka.process :as p])
(require '[cheshire.core :as json])
(require '[clojure.walk])
(require '[babashka.pods :as pods])

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

(let [v (resolve 'a)] (when (bound? v) (p/destroy-tree @v)))

(def a (p/process "./build/example"))

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
    (flush)))

(pods/load-pod "./build/example")

(comment
  (echo/echo 42)
  (echo/echo "hello world")
  (echo/echo ["hello" "world"])

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
