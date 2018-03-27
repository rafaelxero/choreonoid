import os
import cnoid.Base

def loadProject(worldProject, robotProject, perspective = "DoubleSceneViews"):

    directory = os.path.dirname(os.path.realpath(__file__))

    pm = cnoid.Base.ProjectManager.instance()

    pm.loadProject(os.path.join(directory, perspective + ".cnoid"))

    pm.loadProject(os.path.join(directory, worldProject + ".cnoid"))

    worldItem = cnoid.Base.Item.find("World")

    pm.loadProject(os.path.join(directory, robotProject + ".cnoid"), worldItem)

    pm.setCurrentProjectName(worldProject + "-" + robotProject)
