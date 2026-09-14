def can_build(env, platform):
    return not env["disable_3d"]


def configure(env):
    pass


def get_doc_classes():
    return ["Hitscan3D", "HitscanSettings", "MovementHistory3D", "WeaponSimulation", "WeaponSimulationSettings"]


def get_doc_path():
    return "doc_classes"
