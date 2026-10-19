import unreal, traceback
try:
    exec(compile(open(r'C:\UnrealProject\TheLastMessage\Scripts\render_boss_active_core_unreal.py').read(),'render_boss_active_core_unreal.py','exec'))
except Exception:
    unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
