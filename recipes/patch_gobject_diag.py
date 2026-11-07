#!/usr/bin/env python3
# patch_gobject_diag.py -- IN the container: GObject 2.56.4 gobject.c, idempotent. Name the type (and the class flags) when
# g_object_new_internal finds no notify queue, right before its g_assert (nqueue != NULL).
p = "/wk/glib-2.56.4/gobject/gobject.c"
s = open(p, newline="").read()
if "PHE nqueue" not in s:
    msg = ('if (nqueue == NULL) g_printerr ("PHE nqueue NULL: type=%s class=%p flags=%x qdata=%p refcount=%u\\n", '
           'g_type_name (class->g_type_class.g_type), (void *) class, (unsigned) class->flags, (void *) object->qdata, (unsigned) object->ref_count);\n      ')
    s = s.replace("      g_assert (nqueue != NULL);", "      " + msg + "g_assert (nqueue != NULL);", 1)
    open(p, "w", newline="\n").write(s)

# second diagnostic: trace the notify queue of Pango objects (g_object_init, queue creation, queue removal)
s = open(p, newline="").read()
if "PHE init" not in s:
    s = s.replace("  object->ref_count = 1;\n  object->qdata = NULL;\n",
                  "  object->ref_count = 1;\n  object->qdata = NULL;\n"
                  "  if (strncmp (g_type_name (class->g_type_class.g_type), \"Pango\", 5) == 0) g_printerr (\"PHE init %s flags=%x\\n\", g_type_name (class->g_type_class.g_type), (unsigned) class->flags);\n", 1)
    s = s.replace("  g_datalist_id_set_data (&object->qdata, quark_notify_queue, NULL);\n\n  G_UNLOCK(notify_lock);",
                  "  if (strncmp (G_OBJECT_TYPE_NAME (object), \"Pango\", 5) == 0) g_printerr (\"PHE queue removed %s\\n\", G_OBJECT_TYPE_NAME (object));\n"
                  "  g_datalist_id_set_data (&object->qdata, quark_notify_queue, NULL);\n\n  G_UNLOCK(notify_lock);", 1)
    s = s.replace("      nqueue = g_slice_new0 (GObjectNotifyQueue);\n",
                  "      nqueue = g_slice_new0 (GObjectNotifyQueue);\n"
                  "      if (strncmp (G_OBJECT_TYPE_NAME (object), \"Pango\", 5) == 0 || strncmp (G_OBJECT_TYPE_NAME (object), \"GObject\", 7) == 0) g_printerr (\"PHE queue created for %s q=%p quark=%u\\n\", G_OBJECT_TYPE_NAME (object), (void *) nqueue, (unsigned) quark_notify_queue);\n", 1)
    if "#include <string.h>" not in s:
        s = s.replace("#include \"config.h\"\n", "#include \"config.h\"\n#include <string.h>\n", 1)
    open(p, "w", newline="\n").write(s)

# third diagnostic: the registered size of a Pango type right after its instance is created, and where the qdata word sits
s = open(p, newline="").read()
if "PHE new" not in s:
    key = "  object = (GObject *) g_type_create_instance (class->g_type_class.g_type);\n\n  if (CLASS_HAS_PROPS (class))"
    add = ("  object = (GObject *) g_type_create_instance (class->g_type_class.g_type);\n"
           "  if (strncmp (g_type_name (class->g_type_class.g_type), \"Pango\", 5) == 0) { GTypeQuery q_; g_type_query (class->g_type_class.g_type, &q_); "
           "g_printerr (\"PHE new %s instance_size=%u obj=%p &qdata=%p qdata=%p\\n\", g_type_name (class->g_type_class.g_type), q_.instance_size, (void *) object, (void *) &object->qdata, (void *) object->qdata); }\n\n"
           "  if (CLASS_HAS_PROPS (class))")
    if key in s:
        s = s.replace(key, add, 1)
        open(p, "w", newline="\n").write(s)
