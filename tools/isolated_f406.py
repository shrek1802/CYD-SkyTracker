"""Hand-reviewed F406 isolation from a licensed Commons photo.

Works only for the pinned D-IFFF (31697022334).jpg source: a complete F406
in the foreground, another unrelated aircraft in background. Segment the
foreground aircraft so the 320x166 CYD image never shows the other model.
Do not apply to arbitrary photos; the mask geometry is source-specific.
"""
from PIL import Image
import numpy as np
import cv2

POLY=[(80,313),(94,285),(176,271),(235,244),(287,239),(316,229),
      (340,238),(352,258),(598,245),(713,234),(763,143),(862,147),
      (867,168),(824,273),(818,309),(785,341),(677,363),(568,356),
      (505,389),(501,432),(443,434),(424,396),(343,391),(319,424),
      (299,423),(308,365),(258,366),(216,364),(199,418),(153,417),
      (155,360),(85,355)]

def isolate(image: Image.Image, size:tuple[int,int]):
    """Return neutral-background RGB image with full foreground F406."""
    rgb=np.asarray(image.convert("RGB"))
    h,w=rgb.shape[:2]
    # Source on Commons is displayed at 960x642. Refuse unexpected geometry.
    if not (1.44 < float(w)/h < 1.55 and w>=600):
        raise ValueError("F406 source format changed: manual review needed")
    sx,sy=w/960.0,h/642.0
    pts=np.array([(round(x*sx),round(y*sy)) for x,y in POLY],np.int32)
    mask=np.full((h,w),cv2.GC_BGD,np.uint8)
    cv2.fillPoly(mask,[pts],cv2.GC_PR_FGD)
    def ellipse(x,y,a,b):
        cv2.ellipse(mask,(round(x*sx),round(y*sy)),
                    (max(1,round(a*sx)),max(1,round(b*sy))),
                    0,0,360,cv2.GC_FGD,-1)
    ellipse(480,312,255,37)
    ellipse(395,344,110,26)
    ellipse(474,408,15,17)
    ellipse(190,384,8,17)
    cv2.rectangle(mask,(round(765*sx),round(175*sy)),
                  (round(783*sx),round(248*sy)),cv2.GC_FGD,-1)
    bg=np.zeros((1,65),np.float64)
    fg=np.zeros((1,65),np.float64)
    cv2.setRNGSeed(0)
    cv2.grabCut(cv2.cvtColor(rgb,cv2.COLOR_RGB2BGR),
                mask,None,bg,fg,8,cv2.GC_INIT_WITH_MASK)
    alpha=np.where((mask==cv2.GC_FGD)|(mask==cv2.GC_PR_FGD),
                   255,0).astype("uint8")
    alpha=cv2.morphologyEx(alpha,cv2.MORPH_CLOSE,np.ones((3,3),np.uint8))
    alpha=cv2.GaussianBlur(alpha,(5,5),1)
    rgba=Image.fromarray(np.dstack([rgb,alpha]),"RGBA")
    rgba=rgba.crop((round(80*sx),round(125*sy),
                    round(885*sx),round(449*sy)))
    rgba.thumbnail((size[0]-12,size[1]-12),Image.Resampling.LANCZOS)
    out=Image.new("RGBA",size,(16,28,42,255))
    out.alpha_composite(rgba,((size[0]-rgba.width)//2,
                              (size[1]-rgba.height)//2))
    return out.convert("RGB")
