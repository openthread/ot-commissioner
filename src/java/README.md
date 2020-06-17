# OT Commissioner Java

**OT Commissioner Java** binds C++ classes in [include/commissioner](../../include/commissioner) to equivalent Java classes. Instead of crafting JNI and Java classes by hand, we use [SWIG](http://www.swig.org) to generate those Java classes from a defined [interface file](./commissioner.i). This simplifies the maintenance of the Commissioner interface between C++ and Java.

_Note: only synchronized APIs are currently supported in the Java binding, it is encouraged to make asynchronous queries with [Java Executor](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html)._
