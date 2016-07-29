$(document).ready(function() {
    $('a.back').click(function() {
        parent.history.back();
        return false;
    })
    ;
    var id = parseInt(window.location.search.replace(/[^\d.]/g, ''));
    // TODO: assume the data is always sorted, use a binary search to find
    $.each(data.apartments, function(i, obj) {
        if (obj.id === id) {
            $('h1').html(obj.name);
        }

    });

});
