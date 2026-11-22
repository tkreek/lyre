.pragma library

// m:ss, or h:mm:ss for anything an hour or longer.
function time(ms) {
    var total = Math.max(0, Math.floor((ms || 0) / 1000))
    var h = Math.floor(total / 3600), m = Math.floor((total % 3600) / 60), s = total % 60
    var pad = function (n) { return n < 10 ? "0" + n : "" + n }
    return h > 0 ? h + ":" + pad(m) + ":" + pad(s) : m + ":" + pad(s)
}

// Covers are requested in a few fixed sizes, so resizing the window reuses
// what's cached instead of fetching a new size per pixel.
function imageSize(pixels, ratio) {
    var wanted = pixels * (ratio || 1)
    var sizes = [96, 160, 240, 360, 480, 720, 1080]
    for (var i = 0; i < sizes.length; i++)
        if (wanted <= sizes[i])
            return sizes[i]
    return 1080
}
