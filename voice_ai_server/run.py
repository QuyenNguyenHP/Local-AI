import os
from pathlib import Path
import sys
import sysconfig


def configure_cuda_libraries():
    """Restart Python with the project's CUDA libraries visible to the linker."""
    if sys.platform != "linux":
        return
    cuda_root = Path(sysconfig.get_path("purelib")) / "nvidia"
    library_dirs = [
        str(path) for name in ("cublas", "cudnn", "cuda_nvrtc", "cuda_runtime", "nvjitlink")
        if (path := cuda_root / name / "lib").is_dir()
    ]
    if not library_dirs:
        return
    existing = os.environ.get("LD_LIBRARY_PATH", "").split(":")
    if existing[:len(library_dirs)] == library_dirs:
        return
    os.environ["LD_LIBRARY_PATH"] = ":".join(
        library_dirs + [
            path for path in existing
            if path and path not in library_dirs
            and not Path(path).is_relative_to(Path(sys.prefix) / "cuda12")
        ]
    )
    # Updating LD_LIBRARY_PATH within a running process is insufficient for dlopen.
    os.execv(sys.executable, [sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]])

if __name__ == "__main__":
    configure_cuda_libraries()
    os.chdir(Path(__file__).resolve().parent)
    from app.config import get_settings
    import uvicorn

    get_settings()  # Load .env before reading the server's host/port options.
    uvicorn.run("app.main:app", host=os.getenv("HOST", "0.0.0.0"), port=int(os.getenv("PORT", "8000")), reload=os.getenv("RELOAD") == "1")
