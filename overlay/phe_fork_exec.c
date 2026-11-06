
/* ---- Phorepoc: fork_exec_with_pipes over posix_spawn (Open C has no fork). Appended to gspawn.c by patch_glib.sh; the original
 * (renamed fork_exec_with_pipes_unused) stays for reference. Same contract as GLib's, minus what spawning a process cannot do:
 * child_setup runs NOWHERE (it needs the forked child), working_directory is not applied, the search path is not searched
 * (give a path or a ROM executable name), no intermediate child (the pid is the real child's). Pipes: the child's ends are
 * dup2'd onto 0/1/2 through posix_spawn file actions. ---- */
#include <spawn.h>
static gboolean
phe_spawn_fail (GError **error, gint code, const gchar *what, gint errsv)
{
  g_set_error (error, G_SPAWN_ERROR, code, "%s (%s)", what, g_strerror (errsv));
  return FALSE;
}
static gboolean
fork_exec_with_pipes (gboolean              intermediate_child,
                      const gchar          *working_directory,
                      gchar               **argv,
                      gchar               **envp,
                      gboolean              close_descriptors,
                      gboolean              search_path,
                      gboolean              search_path_from_envp,
                      gboolean              stdout_to_null,
                      gboolean              stderr_to_null,
                      gboolean              child_inherits_stdin,
                      gboolean              file_and_argv_zero,
                      gboolean              cloexec_pipes,
                      GSpawnChildSetupFunc  child_setup,
                      gpointer              user_data,
                      GPid                 *child_pid,
                      gint                 *standard_input,
                      gint                 *standard_output,
                      gint                 *standard_error,
                      GError              **error)
{
  gint in_p[2] = { -1, -1 }, out_p[2] = { -1, -1 }, err_p[2] = { -1, -1 };
  posix_spawn_file_actions_t fa;
  pid_t pid = 0;
  gint r;
  gchar **args = file_and_argv_zero ? argv + 1 : argv;
  extern char **environ;
  (void) intermediate_child; (void) working_directory; (void) close_descriptors; (void) search_path; (void) search_path_from_envp;
  (void) cloexec_pipes; (void) child_setup; (void) user_data;
  if (standard_input && !g_unix_open_pipe (in_p, 0, error)) return FALSE;
  if (standard_output && !g_unix_open_pipe (out_p, 0, error)) goto fail;
  if (standard_error && !g_unix_open_pipe (err_p, 0, error)) goto fail;
  posix_spawn_file_actions_init (&fa);
  if (standard_input) posix_spawn_file_actions_adddup2 (&fa, in_p[0], 0);
  if (standard_output) posix_spawn_file_actions_adddup2 (&fa, out_p[1], 1);
  if (standard_error) posix_spawn_file_actions_adddup2 (&fa, err_p[1], 2);
  (void) stdout_to_null; (void) stderr_to_null; (void) child_inherits_stdin;
  r = posix_spawn (&pid, argv[0], &fa, NULL, args, envp ? envp : environ);
  posix_spawn_file_actions_destroy (&fa);
  if (r != 0) { phe_spawn_fail (error, G_SPAWN_ERROR_FAILED, "Failed to spawn", r); goto fail; }
  if (in_p[0] >= 0) close (in_p[0]);
  if (out_p[1] >= 0) close (out_p[1]);
  if (err_p[1] >= 0) close (err_p[1]);
  if (child_pid) *child_pid = pid;
  if (standard_input) *standard_input = in_p[1];
  if (standard_output) *standard_output = out_p[0];
  if (standard_error) *standard_error = err_p[0];
  return TRUE;
fail:
  { gint i; gint *all[3] = { in_p, out_p, err_p }; for (i = 0; i < 3; i++) { if (all[i][0] >= 0) close (all[i][0]); if (all[i][1] >= 0) close (all[i][1]); } }
  return FALSE;
}
