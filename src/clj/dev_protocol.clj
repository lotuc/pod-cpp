(ns clj.dev-protocol)

(require '[clojure.java.io :as io])
(require '[bencode.core :as b])
(require '[babashka.process :as p])
(require '[cheshire.core :as json])
(require '[clojure.walk])

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
  dev-process

  (to-netstring {:op "describe"})
  "d2:op8:describee"

  (shut-dev-process)
  (restart-dev-process)
  (w {:op "describe"})

  (w {:op "load-ns" :ns "test-pod-defer"})

  (to-netstring {:op "load-ns" :ns "test-pod-defer" :id "42"})
  "d2:id2:422:ns14:test-pod-defer2:op7:load-nse"
  "d2:ns14:test-pod-defer2:op7:load-nse"

  (w {:op "invoke" :id "42" :var "test-pod/echo"
      :args (json/generate-string ["hello world"])})

  (w {:op "invoke" :id "42" :var "lotuc.babashka.pods/pendings"
      :args (json/generate-string [])})

  (w {:op "invoke" :id "42" :var "test_pod/echo"
      :args (json/generate-string [])})

  (w {:op "invoke" :id "42" :var "test_pod/error"
      :args (json/generate-string [])})

  (to-netstring {:op "describe"})
  "d2:op8:describee"

  (to-netstring {:op "invoke" :id "42" :var "test_pod/echo"
                 :args (json/generate-string [])})
  "d4:args2:[]2:id2:422:op6:invoke3:var13:test_pod/echoe"

  (to-netstring {:op "invoke" :id "42" :var "test_pod/error"
                 :args (json/generate-string [])})
  "d4:args2:[]2:id2:422:op6:invoke3:var14:test_pod/errore"

  #_())

(to-netstring {:op "invoke" :id "42" :var "lotuc.babashka.pods/pendings"
               :args (json/generate-string [])})
"d4:args2:[]2:id2:422:op6:invoke3:var28:lotuc.babashka.pods/pendingse"

(to-netstring {:op "invoke" :id "42" :var "test-pod/sleep"
               :args (json/generate-string [10000])})
"d4:args7:[10000]2:id2:422:op6:invoke3:var14:test-pod/sleepe"
